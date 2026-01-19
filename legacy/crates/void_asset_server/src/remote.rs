//! Remote asset source for connecting to void-assets server
//!
//! Provides WebSocket-based real-time asset updates from a remote void-assets server.

use std::collections::HashMap;
use std::sync::Arc;

use futures_util::{SinkExt, StreamExt};
use parking_lot::RwLock;
use tokio::sync::mpsc;
use tokio_tungstenite::{connect_async, tungstenite::Message};
use url::Url;

use crate::loaders::{SceneAsset, SceneLoader};

/// Events received from the remote void-assets server
#[derive(Debug, Clone)]
pub enum RemoteEvent {
    /// Successfully connected to the server
    Connected { project_id: String },
    /// Disconnected from the server
    Disconnected { reason: String },
    /// A scene was created
    SceneCreated { scene_id: String, name: String },
    /// A scene was updated
    SceneUpdated { scene_id: String, version: u32 },
    /// A scene was deleted
    SceneDeleted { scene_id: String },
    /// An asset was uploaded
    AssetUploaded {
        asset_id: String,
        filename: String,
        asset_type: String,
    },
    /// An asset was deleted
    AssetDeleted { asset_id: String },
    /// Error occurred
    Error { message: String },
}

/// Configuration for the remote asset source
#[derive(Debug, Clone)]
pub struct RemoteConfig {
    /// Base URL for the void-assets REST API (e.g., "http://localhost:3001")
    pub base_url: String,
    /// Project ID to subscribe to
    pub project_id: String,
    /// Whether to automatically reconnect on disconnect
    pub auto_reconnect: bool,
    /// Reconnect delay in milliseconds
    pub reconnect_delay_ms: u64,
}

impl Default for RemoteConfig {
    fn default() -> Self {
        Self {
            base_url: "http://localhost:3001".to_string(),
            project_id: String::new(),
            auto_reconnect: true,
            reconnect_delay_ms: 3000,
        }
    }
}

/// State shared between the main thread and the WebSocket task
struct SharedState {
    connected: bool,
    pending_events: Vec<RemoteEvent>,
    cached_scenes: HashMap<String, SceneAsset>,
    cached_assets: HashMap<String, Vec<u8>>,
}

/// Remote asset source that connects to a void-assets server
pub struct RemoteAssetSource {
    config: RemoteConfig,
    state: Arc<RwLock<SharedState>>,
    runtime: Option<tokio::runtime::Runtime>,
    shutdown_tx: Option<mpsc::Sender<()>>,
}

impl RemoteAssetSource {
    /// Create a new remote asset source with the given configuration
    pub fn new(config: RemoteConfig) -> Self {
        Self {
            config,
            state: Arc::new(RwLock::new(SharedState {
                connected: false,
                pending_events: Vec::new(),
                cached_scenes: HashMap::new(),
                cached_assets: HashMap::new(),
            })),
            runtime: None,
            shutdown_tx: None,
        }
    }

    /// Connect to the void-assets server
    pub fn connect(&mut self) -> Result<(), String> {
        if self.runtime.is_some() {
            return Err("Already connected".to_string());
        }

        let runtime = tokio::runtime::Builder::new_multi_thread()
            .worker_threads(2)
            .enable_all()
            .build()
            .map_err(|e| format!("Failed to create runtime: {}", e))?;

        let (shutdown_tx, shutdown_rx) = mpsc::channel(1);
        let state = self.state.clone();
        let config = self.config.clone();

        runtime.spawn(async move {
            run_websocket_loop(config, state, shutdown_rx).await;
        });

        self.runtime = Some(runtime);
        self.shutdown_tx = Some(shutdown_tx);

        log::info!(
            "RemoteAssetSource: Connecting to {} for project {}",
            self.config.base_url,
            self.config.project_id
        );

        Ok(())
    }

    /// Disconnect from the server
    pub fn disconnect(&mut self) {
        if let Some(tx) = self.shutdown_tx.take() {
            let _ = tx.blocking_send(());
        }
        if let Some(runtime) = self.runtime.take() {
            runtime.shutdown_background();
        }
        self.state.write().connected = false;
        log::info!("RemoteAssetSource: Disconnected");
    }

    /// Check if connected to the server
    pub fn is_connected(&self) -> bool {
        self.state.read().connected
    }

    /// Poll for pending events (call each frame)
    pub fn poll(&mut self) -> Vec<RemoteEvent> {
        std::mem::take(&mut self.state.write().pending_events)
    }

    /// Fetch a scene from the server (blocking)
    pub fn fetch_scene(&self, scene_id: &str) -> Result<SceneAsset, String> {
        // Check cache first
        if let Some(scene) = self.state.read().cached_scenes.get(scene_id) {
            return Ok(scene.clone());
        }

        let runtime = self
            .runtime
            .as_ref()
            .ok_or("Not connected")?;

        let url = format!(
            "{}/api/projects/{}/scenes/{}",
            self.config.base_url, self.config.project_id, scene_id
        );

        let result = runtime.block_on(async {
            fetch_json::<serde_json::Value>(&url).await
        })?;

        // Convert to SceneAsset
        let scene = convert_to_scene_asset(result)?;

        // Cache it
        self.state
            .write()
            .cached_scenes
            .insert(scene_id.to_string(), scene.clone());

        Ok(scene)
    }

    /// Fetch an asset by hash (blocking)
    pub fn fetch_asset(&self, hash: &str) -> Result<Vec<u8>, String> {
        // Check cache first
        if let Some(data) = self.state.read().cached_assets.get(hash) {
            return Ok(data.clone());
        }

        let runtime = self
            .runtime
            .as_ref()
            .ok_or("Not connected")?;

        let url = format!("{}/api/assets/{}", self.config.base_url, hash);

        let data = runtime.block_on(async { fetch_bytes(&url).await })?;

        // Cache it
        self.state
            .write()
            .cached_assets
            .insert(hash.to_string(), data.clone());

        Ok(data)
    }

    /// List all scenes in the project (blocking)
    pub fn list_scenes(&self) -> Result<Vec<SceneInfo>, String> {
        let runtime = self
            .runtime
            .as_ref()
            .ok_or("Not connected")?;

        let url = format!(
            "{}/api/projects/{}/scenes",
            self.config.base_url, self.config.project_id
        );

        let result = runtime.block_on(async {
            fetch_json::<Vec<serde_json::Value>>(&url).await
        })?;

        let scenes = result
            .into_iter()
            .filter_map(|v| {
                Some(SceneInfo {
                    id: v.get("id")?.as_str()?.to_string(),
                    name: v.get("name")?.as_str()?.to_string(),
                    version: v.get("version")?.as_u64()? as u32,
                })
            })
            .collect();

        Ok(scenes)
    }

    /// List all assets in the project (blocking)
    pub fn list_assets(&self, asset_type: Option<&str>) -> Result<Vec<AssetInfo>, String> {
        let runtime = self
            .runtime
            .as_ref()
            .ok_or("Not connected")?;

        let mut url = format!(
            "{}/api/assets/projects/{}",
            self.config.base_url, self.config.project_id
        );

        if let Some(t) = asset_type {
            url.push_str(&format!("?type={}", t));
        }

        let result = runtime.block_on(async {
            fetch_json::<Vec<serde_json::Value>>(&url).await
        })?;

        let assets = result
            .into_iter()
            .filter_map(|v| {
                Some(AssetInfo {
                    id: v.get("id")?.as_str()?.to_string(),
                    hash: v.get("hash")?.as_str()?.to_string(),
                    filename: v.get("filename")?.as_str()?.to_string(),
                    asset_type: v.get("assetType")?.as_str()?.to_string(),
                    size: v.get("size")?.as_u64()? as usize,
                })
            })
            .collect();

        Ok(assets)
    }

    /// Invalidate cached scene (forces re-fetch on next access)
    pub fn invalidate_scene(&self, scene_id: &str) {
        self.state.write().cached_scenes.remove(scene_id);
    }

    /// Invalidate cached asset (forces re-fetch on next access)
    pub fn invalidate_asset(&self, hash: &str) {
        self.state.write().cached_assets.remove(hash);
    }

    /// Clear all caches
    pub fn clear_cache(&self) {
        let mut state = self.state.write();
        state.cached_scenes.clear();
        state.cached_assets.clear();
    }
}

impl Drop for RemoteAssetSource {
    fn drop(&mut self) {
        self.disconnect();
    }
}

/// Scene info from the server
#[derive(Debug, Clone)]
pub struct SceneInfo {
    pub id: String,
    pub name: String,
    pub version: u32,
}

/// Asset info from the server
#[derive(Debug, Clone)]
pub struct AssetInfo {
    pub id: String,
    pub hash: String,
    pub filename: String,
    pub asset_type: String,
    pub size: usize,
}

// ============================================================================
// Internal async functions
// ============================================================================

async fn run_websocket_loop(
    config: RemoteConfig,
    state: Arc<RwLock<SharedState>>,
    mut shutdown_rx: mpsc::Receiver<()>,
) {
    loop {
        // Build WebSocket URL
        let ws_url = config
            .base_url
            .replace("http://", "ws://")
            .replace("https://", "wss://");
        let ws_url = format!("{}/ws/{}", ws_url, config.project_id);

        log::info!("RemoteAssetSource: Connecting to WebSocket at {}", ws_url);

        match connect_async(&ws_url).await {
            Ok((ws_stream, _)) => {
                state.write().connected = true;
                state.write().pending_events.push(RemoteEvent::Connected {
                    project_id: config.project_id.clone(),
                });

                log::info!("RemoteAssetSource: WebSocket connected");

                let (mut write, mut read) = ws_stream.split();

                loop {
                    tokio::select! {
                        _ = shutdown_rx.recv() => {
                            log::info!("RemoteAssetSource: Shutdown requested");
                            let _ = write.close().await;
                            return;
                        }
                        msg = read.next() => {
                            match msg {
                                Some(Ok(Message::Text(text))) => {
                                    if let Ok(json) = serde_json::from_str::<serde_json::Value>(&text) {
                                        if let Some(event) = parse_ws_event(&json) {
                                            // Invalidate cache for updated resources
                                            match &event {
                                                RemoteEvent::SceneUpdated { scene_id, .. } |
                                                RemoteEvent::SceneDeleted { scene_id } => {
                                                    state.write().cached_scenes.remove(scene_id);
                                                }
                                                _ => {}
                                            }
                                            state.write().pending_events.push(event);
                                        }
                                    }
                                }
                                Some(Ok(Message::Ping(data))) => {
                                    let _ = write.send(Message::Pong(data)).await;
                                }
                                Some(Ok(Message::Close(_))) => {
                                    log::info!("RemoteAssetSource: Server closed connection");
                                    break;
                                }
                                Some(Err(e)) => {
                                    log::error!("RemoteAssetSource: WebSocket error: {}", e);
                                    break;
                                }
                                None => {
                                    log::info!("RemoteAssetSource: WebSocket stream ended");
                                    break;
                                }
                                _ => {}
                            }
                        }
                    }
                }

                state.write().connected = false;
                state.write().pending_events.push(RemoteEvent::Disconnected {
                    reason: "Connection closed".to_string(),
                });
            }
            Err(e) => {
                log::error!("RemoteAssetSource: Failed to connect: {}", e);
                state.write().pending_events.push(RemoteEvent::Error {
                    message: format!("Connection failed: {}", e),
                });
            }
        }

        if !config.auto_reconnect {
            return;
        }

        // Check for shutdown before reconnecting
        tokio::select! {
            _ = shutdown_rx.recv() => {
                return;
            }
            _ = tokio::time::sleep(std::time::Duration::from_millis(config.reconnect_delay_ms)) => {
                log::info!("RemoteAssetSource: Attempting to reconnect...");
            }
        }
    }
}

fn parse_ws_event(json: &serde_json::Value) -> Option<RemoteEvent> {
    let event_type = json.get("type")?.as_str()?;

    match event_type {
        "scene:created" => Some(RemoteEvent::SceneCreated {
            scene_id: json.get("sceneId")?.as_str()?.to_string(),
            name: json.get("name")?.as_str()?.to_string(),
        }),
        "scene:updated" => Some(RemoteEvent::SceneUpdated {
            scene_id: json.get("sceneId")?.as_str()?.to_string(),
            version: json.get("version")?.as_u64()? as u32,
        }),
        "scene:deleted" => Some(RemoteEvent::SceneDeleted {
            scene_id: json.get("sceneId")?.as_str()?.to_string(),
        }),
        "asset:uploaded" => Some(RemoteEvent::AssetUploaded {
            asset_id: json.get("assetId")?.as_str()?.to_string(),
            filename: json.get("filename")?.as_str()?.to_string(),
            asset_type: json.get("assetType")?.as_str()?.to_string(),
        }),
        "asset:deleted" => Some(RemoteEvent::AssetDeleted {
            asset_id: json.get("assetId")?.as_str()?.to_string(),
        }),
        _ => None,
    }
}

async fn fetch_json<T: serde::de::DeserializeOwned>(url: &str) -> Result<T, String> {
    // Use a simple HTTP client without reqwest to reduce dependencies
    // For now, use tokio-tungstenite's built-in HTTP support
    let url = Url::parse(url).map_err(|e| format!("Invalid URL: {}", e))?;

    let host = url.host_str().ok_or("No host in URL")?;
    let port = url.port().unwrap_or(if url.scheme() == "https" { 443 } else { 80 });
    let path = url.path();
    let query = url.query().map(|q| format!("?{}", q)).unwrap_or_default();

    let addr = format!("{}:{}", host, port);
    let stream = tokio::net::TcpStream::connect(&addr)
        .await
        .map_err(|e| format!("Connection failed: {}", e))?;

    let request = format!(
        "GET {}{} HTTP/1.1\r\nHost: {}\r\nConnection: close\r\n\r\n",
        path, query, host
    );

    use tokio::io::{AsyncReadExt, AsyncWriteExt};
    let mut stream = stream;
    stream
        .write_all(request.as_bytes())
        .await
        .map_err(|e| format!("Write failed: {}", e))?;

    let mut response = Vec::new();
    stream
        .read_to_end(&mut response)
        .await
        .map_err(|e| format!("Read failed: {}", e))?;

    let response = String::from_utf8_lossy(&response);
    let body_start = response
        .find("\r\n\r\n")
        .ok_or("Invalid HTTP response")?
        + 4;
    let body = &response[body_start..];

    serde_json::from_str(body).map_err(|e| format!("JSON parse error: {}", e))
}

async fn fetch_bytes(url: &str) -> Result<Vec<u8>, String> {
    let url = Url::parse(url).map_err(|e| format!("Invalid URL: {}", e))?;

    let host = url.host_str().ok_or("No host in URL")?;
    let port = url.port().unwrap_or(if url.scheme() == "https" { 443 } else { 80 });
    let path = url.path();

    let addr = format!("{}:{}", host, port);
    let stream = tokio::net::TcpStream::connect(&addr)
        .await
        .map_err(|e| format!("Connection failed: {}", e))?;

    let request = format!(
        "GET {} HTTP/1.1\r\nHost: {}\r\nConnection: close\r\n\r\n",
        path, host
    );

    use tokio::io::{AsyncReadExt, AsyncWriteExt};
    let mut stream = stream;
    stream
        .write_all(request.as_bytes())
        .await
        .map_err(|e| format!("Write failed: {}", e))?;

    let mut response = Vec::new();
    stream
        .read_to_end(&mut response)
        .await
        .map_err(|e| format!("Read failed: {}", e))?;

    // Find body start (after \r\n\r\n)
    let mut body_start = 0;
    for i in 0..response.len().saturating_sub(3) {
        if &response[i..i + 4] == b"\r\n\r\n" {
            body_start = i + 4;
            break;
        }
    }

    Ok(response[body_start..].to_vec())
}

fn convert_to_scene_asset(json: serde_json::Value) -> Result<SceneAsset, String> {
    let json_str = serde_json::to_string(&json).map_err(|e| e.to_string())?;
    SceneLoader::load(json_str.as_bytes(), "remote")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_scene_updated() {
        let json = serde_json::json!({
            "type": "scene:updated",
            "sceneId": "abc123",
            "version": 5
        });

        let event = parse_ws_event(&json).unwrap();
        match event {
            RemoteEvent::SceneUpdated { scene_id, version } => {
                assert_eq!(scene_id, "abc123");
                assert_eq!(version, 5);
            }
            _ => panic!("Wrong event type"),
        }
    }

    #[test]
    fn test_parse_asset_uploaded() {
        let json = serde_json::json!({
            "type": "asset:uploaded",
            "assetId": "xyz789",
            "filename": "texture.png",
            "assetType": "texture"
        });

        let event = parse_ws_event(&json).unwrap();
        match event {
            RemoteEvent::AssetUploaded {
                asset_id,
                filename,
                asset_type,
            } => {
                assert_eq!(asset_id, "xyz789");
                assert_eq!(filename, "texture.png");
                assert_eq!(asset_type, "texture");
            }
            _ => panic!("Wrong event type"),
        }
    }
}
