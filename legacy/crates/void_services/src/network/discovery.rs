//! World Discovery
//!
//! Provides world discovery, registration, and browsing functionality.
//! Supports multiple discovery protocols: central registry, mDNS, and DHT.

use std::collections::HashMap;
use std::time::{Duration, Instant};

use serde::{Deserialize, Serialize};
use uuid::Uuid;

use super::connection::WorldId;
use super::{NetworkError, NetworkResult};

/// Unique capability identifier
#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct CapabilityId(String);

impl CapabilityId {
    /// Create a new capability ID
    pub fn new(id: impl Into<String>) -> Self {
        Self(id.into())
    }

    /// Get the ID string
    pub fn as_str(&self) -> &str {
        &self.0
    }
}

impl std::fmt::Display for CapabilityId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}

/// World metadata for display and filtering
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct WorldMetadata {
    /// Short description of the world
    pub description: String,
    /// URL to thumbnail image
    pub thumbnail_url: Option<String>,
    /// Tags for categorization
    pub tags: Vec<String>,
    /// World category
    pub category: Option<String>,
    /// Content rating (e.g., "Everyone", "Teen", "Mature")
    pub content_rating: Option<String>,
    /// Custom key-value metadata
    pub custom: HashMap<String, String>,
}

impl WorldMetadata {
    /// Create new metadata with a description
    pub fn new(description: impl Into<String>) -> Self {
        Self {
            description: description.into(),
            ..Default::default()
        }
    }

    /// Add a tag
    pub fn with_tag(mut self, tag: impl Into<String>) -> Self {
        self.tags.push(tag.into());
        self
    }

    /// Add multiple tags
    pub fn with_tags(mut self, tags: impl IntoIterator<Item = impl Into<String>>) -> Self {
        self.tags.extend(tags.into_iter().map(Into::into));
        self
    }

    /// Set the thumbnail URL
    pub fn with_thumbnail(mut self, url: impl Into<String>) -> Self {
        self.thumbnail_url = Some(url.into());
        self
    }

    /// Set the category
    pub fn with_category(mut self, category: impl Into<String>) -> Self {
        self.category = Some(category.into());
        self
    }

    /// Set the content rating
    pub fn with_rating(mut self, rating: impl Into<String>) -> Self {
        self.content_rating = Some(rating.into());
        self
    }

    /// Add custom metadata
    pub fn with_custom(mut self, key: impl Into<String>, value: impl Into<String>) -> Self {
        self.custom.insert(key.into(), value.into());
        self
    }

    /// Check if world has a specific tag
    pub fn has_tag(&self, tag: &str) -> bool {
        self.tags.iter().any(|t| t.eq_ignore_ascii_case(tag))
    }
}

/// Information about a world server
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WorldServerInfo {
    /// Unique world identifier
    pub world_id: WorldId,
    /// Human-readable name
    pub name: String,
    /// Server address (WebSocket URL)
    pub address: String,
    /// Current player population
    pub population: u32,
    /// Maximum player capacity
    pub capacity: u32,
    /// World metadata
    pub metadata: WorldMetadata,
    /// Required capabilities to join
    pub required_capabilities: Vec<CapabilityId>,
    /// When this info was last updated
    #[serde(skip)]
    pub last_update: Option<Instant>,
    /// Ping latency to this server (if measured)
    #[serde(skip)]
    pub ping_ms: Option<u32>,
    /// Is the server online
    pub online: bool,
    /// Server version
    pub version: String,
    /// Region/location identifier
    pub region: Option<String>,
}

impl WorldServerInfo {
    /// Create new world server info
    pub fn new(name: impl Into<String>, address: impl Into<String>) -> Self {
        Self {
            world_id: WorldId::new(),
            name: name.into(),
            address: address.into(),
            population: 0,
            capacity: 100,
            metadata: WorldMetadata::default(),
            required_capabilities: Vec::new(),
            last_update: Some(Instant::now()),
            ping_ms: None,
            online: true,
            version: "1.0.0".to_string(),
            region: None,
        }
    }

    /// Set the world ID
    pub fn with_id(mut self, id: WorldId) -> Self {
        self.world_id = id;
        self
    }

    /// Set the capacity
    pub fn with_capacity(mut self, capacity: u32) -> Self {
        self.capacity = capacity;
        self
    }

    /// Set the population
    pub fn with_population(mut self, population: u32) -> Self {
        self.population = population;
        self
    }

    /// Set the metadata
    pub fn with_metadata(mut self, metadata: WorldMetadata) -> Self {
        self.metadata = metadata;
        self
    }

    /// Add a required capability
    pub fn with_required_capability(mut self, capability: CapabilityId) -> Self {
        self.required_capabilities.push(capability);
        self
    }

    /// Set the region
    pub fn with_region(mut self, region: impl Into<String>) -> Self {
        self.region = Some(region.into());
        self
    }

    /// Check if the world is full
    pub fn is_full(&self) -> bool {
        self.population >= self.capacity
    }

    /// Get the fill percentage (0.0 - 1.0)
    pub fn fill_percentage(&self) -> f32 {
        if self.capacity == 0 {
            return 0.0;
        }
        self.population as f32 / self.capacity as f32
    }

    /// Check if info is stale (older than given duration)
    pub fn is_stale(&self, max_age: Duration) -> bool {
        match self.last_update {
            Some(time) => time.elapsed() > max_age,
            None => true,
        }
    }
}

/// Configuration for hosting a world
#[derive(Debug, Clone)]
pub struct WorldConfig {
    /// World name
    pub name: String,
    /// World description
    pub description: String,
    /// Maximum players
    pub max_players: u32,
    /// World metadata
    pub metadata: WorldMetadata,
    /// Required capabilities
    pub required_capabilities: Vec<CapabilityId>,
    /// Whether the world is public (discoverable)
    pub public: bool,
    /// Password for private worlds
    pub password: Option<String>,
    /// Server port
    pub port: u16,
    /// Region identifier
    pub region: Option<String>,
}

impl Default for WorldConfig {
    fn default() -> Self {
        Self {
            name: "New World".to_string(),
            description: String::new(),
            max_players: 100,
            metadata: WorldMetadata::default(),
            required_capabilities: Vec::new(),
            public: true,
            password: None,
            port: 8080,
            region: None,
        }
    }
}

impl WorldConfig {
    /// Create a new world config with a name
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            ..Default::default()
        }
    }

    /// Set the description
    pub fn with_description(mut self, description: impl Into<String>) -> Self {
        self.description = description.into();
        self
    }

    /// Set the max players
    pub fn with_max_players(mut self, max: u32) -> Self {
        self.max_players = max;
        self
    }

    /// Make the world private with a password
    pub fn with_password(mut self, password: impl Into<String>) -> Self {
        self.public = false;
        self.password = Some(password.into());
        self
    }
}

/// Discovery protocol types
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DiscoveryMethod {
    /// Central HTTP registry
    CentralRegistry,
    /// Local network mDNS
    LocalMdns,
    /// Distributed hash table
    Dht,
    /// Direct address (no discovery)
    Direct,
}

/// Discovery protocol configuration
#[derive(Debug, Clone)]
pub struct DiscoveryConfig {
    /// Central registry URL
    pub central_registry_url: Option<String>,
    /// mDNS service name
    pub mdns_service_name: String,
    /// Discovery timeout
    pub timeout: Duration,
    /// Enable local network discovery
    pub enable_local: bool,
    /// Enable DHT discovery
    pub enable_dht: bool,
    /// Cache duration for discovered worlds
    pub cache_duration: Duration,
}

impl Default for DiscoveryConfig {
    fn default() -> Self {
        Self {
            central_registry_url: None,
            mdns_service_name: "_metaverse._tcp.local".to_string(),
            timeout: Duration::from_secs(5),
            enable_local: true,
            enable_dht: false, // Not implemented yet
            cache_duration: Duration::from_secs(60),
        }
    }
}

/// World registry - discovers and manages world servers
pub struct WorldRegistry {
    /// Configuration
    config: DiscoveryConfig,
    /// Known world servers (cached)
    servers: HashMap<WorldId, WorldServerInfo>,
    /// Favorite worlds
    favorites: Vec<WorldId>,
    /// Recently visited worlds
    recent: Vec<WorldId>,
    /// Local address for hosting
    local_address: Option<String>,
}

impl WorldRegistry {
    /// Create a new world registry
    pub fn new() -> Self {
        Self::with_config(DiscoveryConfig::default())
    }

    /// Create with custom configuration
    pub fn with_config(config: DiscoveryConfig) -> Self {
        Self {
            config,
            servers: HashMap::new(),
            favorites: Vec::new(),
            recent: Vec::new(),
            local_address: None,
        }
    }

    /// Set the local address for hosting
    pub fn set_local_address(&mut self, address: impl Into<String>) {
        self.local_address = Some(address.into());
    }

    /// Get the local address
    pub fn local_address(&self) -> Option<&str> {
        self.local_address.as_deref()
    }

    /// Discover available worlds
    pub async fn discover(&mut self) -> Vec<WorldServerInfo> {
        let mut worlds = Vec::new();

        // Query central registry
        if let Some(ref url) = self.config.central_registry_url {
            match self.query_central(url).await {
                Ok(central_worlds) => worlds.extend(central_worlds),
                Err(e) => log::warn!("Central registry query failed: {}", e),
            }
        }

        // Local network discovery
        if self.config.enable_local {
            match self.query_local().await {
                Ok(local_worlds) => worlds.extend(local_worlds),
                Err(e) => log::debug!("Local discovery failed: {}", e),
            }
        }

        // DHT discovery (future)
        if self.config.enable_dht {
            match self.query_dht().await {
                Ok(dht_worlds) => worlds.extend(dht_worlds),
                Err(e) => log::debug!("DHT discovery failed: {}", e),
            }
        }

        // Update cache
        for world in &worlds {
            self.servers.insert(world.world_id, world.clone());
        }

        // Clean up stale entries
        self.cleanup_stale();

        worlds
    }

    /// Query central registry API
    async fn query_central(&self, _url: &str) -> NetworkResult<Vec<WorldServerInfo>> {
        // TODO: Implement HTTP API query
        // For now, return empty list
        log::debug!("Central registry query not yet implemented");
        Ok(Vec::new())
    }

    /// Query local network via mDNS
    async fn query_local(&self) -> NetworkResult<Vec<WorldServerInfo>> {
        // TODO: Implement mDNS discovery
        log::debug!("Local mDNS discovery not yet implemented");
        Ok(Vec::new())
    }

    /// Query DHT for worlds
    async fn query_dht(&self) -> NetworkResult<Vec<WorldServerInfo>> {
        // TODO: Implement DHT discovery
        log::debug!("DHT discovery not yet implemented");
        Ok(Vec::new())
    }

    /// Register a world with the discovery services
    pub async fn host_world(&mut self, config: WorldConfig) -> NetworkResult<WorldId> {
        let world_id = WorldId::new();

        let local_address = self.local_address.clone().ok_or_else(|| {
            NetworkError::Internal("Local address not set".to_string())
        })?;

        let address = format!("ws://{}:{}", local_address, config.port);

        let server_info = WorldServerInfo::new(&config.name, address)
            .with_id(world_id)
            .with_capacity(config.max_players)
            .with_metadata(config.metadata);

        // Register with central registry
        if let Some(ref url) = self.config.central_registry_url {
            if let Err(e) = self.register_central(url, &server_info).await {
                log::warn!("Failed to register with central registry: {}", e);
            }
        }

        // Announce on local network
        if self.config.enable_local {
            if let Err(e) = self.announce_local(&server_info).await {
                log::debug!("Failed to announce locally: {}", e);
            }
        }

        // Add to local cache
        self.servers.insert(world_id, server_info);

        log::info!("World hosted: {} ({})", config.name, world_id);
        Ok(world_id)
    }

    /// Register with central registry
    async fn register_central(
        &self,
        _url: &str,
        _info: &WorldServerInfo,
    ) -> NetworkResult<()> {
        // TODO: Implement HTTP API registration
        log::debug!("Central registry registration not yet implemented");
        Ok(())
    }

    /// Announce world on local network
    async fn announce_local(&self, _info: &WorldServerInfo) -> NetworkResult<()> {
        // TODO: Implement mDNS announcement
        log::debug!("Local announcement not yet implemented");
        Ok(())
    }

    /// Unregister a hosted world
    pub async fn unhost_world(&mut self, world_id: WorldId) -> NetworkResult<()> {
        self.servers.remove(&world_id);

        // Unregister from central registry
        if let Some(ref url) = self.config.central_registry_url {
            if let Err(e) = self.unregister_central(url, world_id).await {
                log::warn!("Failed to unregister from central registry: {}", e);
            }
        }

        log::info!("World unhosted: {}", world_id);
        Ok(())
    }

    /// Unregister from central registry
    async fn unregister_central(&self, _url: &str, _world_id: WorldId) -> NetworkResult<()> {
        // TODO: Implement HTTP API unregistration
        Ok(())
    }

    /// Get a known world by ID
    pub fn get_world(&self, world_id: &WorldId) -> Option<&WorldServerInfo> {
        self.servers.get(world_id)
    }

    /// Get all known worlds
    pub fn all_worlds(&self) -> Vec<&WorldServerInfo> {
        self.servers.values().collect()
    }

    /// Get worlds matching a filter
    pub fn find_worlds<F>(&self, filter: F) -> Vec<&WorldServerInfo>
    where
        F: Fn(&WorldServerInfo) -> bool,
    {
        self.servers.values().filter(|w| filter(w)).collect()
    }

    /// Find worlds by tag
    pub fn find_by_tag(&self, tag: &str) -> Vec<&WorldServerInfo> {
        self.find_worlds(|w| w.metadata.has_tag(tag))
    }

    /// Find worlds with available space
    pub fn find_available(&self) -> Vec<&WorldServerInfo> {
        self.find_worlds(|w| w.online && !w.is_full())
    }

    /// Add a world to favorites
    pub fn add_favorite(&mut self, world_id: WorldId) {
        if !self.favorites.contains(&world_id) {
            self.favorites.push(world_id);
        }
    }

    /// Remove a world from favorites
    pub fn remove_favorite(&mut self, world_id: &WorldId) {
        self.favorites.retain(|id| id != world_id);
    }

    /// Get favorite worlds
    pub fn favorites(&self) -> Vec<&WorldServerInfo> {
        self.favorites
            .iter()
            .filter_map(|id| self.servers.get(id))
            .collect()
    }

    /// Record a world as recently visited
    pub fn record_visit(&mut self, world_id: WorldId) {
        // Remove if already in list
        self.recent.retain(|id| id != &world_id);
        // Add to front
        self.recent.insert(0, world_id);
        // Keep only last 20
        self.recent.truncate(20);
    }

    /// Get recently visited worlds
    pub fn recent(&self) -> Vec<&WorldServerInfo> {
        self.recent
            .iter()
            .filter_map(|id| self.servers.get(id))
            .collect()
    }

    /// Add a world directly (for direct connect)
    pub fn add_direct(&mut self, info: WorldServerInfo) {
        self.servers.insert(info.world_id, info);
    }

    /// Clean up stale server entries
    fn cleanup_stale(&mut self) {
        let max_age = self.config.cache_duration;
        self.servers.retain(|_, info| !info.is_stale(max_age));
    }

    /// Get registry statistics
    pub fn stats(&self) -> RegistryStats {
        let total = self.servers.len();
        let online = self.servers.values().filter(|s| s.online).count();
        let total_players: u32 = self.servers.values().map(|s| s.population).sum();
        let total_capacity: u32 = self.servers.values().map(|s| s.capacity).sum();

        RegistryStats {
            total_worlds: total,
            online_worlds: online,
            total_players,
            total_capacity,
            favorites_count: self.favorites.len(),
            recent_count: self.recent.len(),
        }
    }
}

impl Default for WorldRegistry {
    fn default() -> Self {
        Self::new()
    }
}

/// Registry statistics
#[derive(Debug, Clone)]
pub struct RegistryStats {
    /// Total known worlds
    pub total_worlds: usize,
    /// Currently online worlds
    pub online_worlds: usize,
    /// Total players across all worlds
    pub total_players: u32,
    /// Total capacity across all worlds
    pub total_capacity: u32,
    /// Number of favorites
    pub favorites_count: usize,
    /// Number of recent visits
    pub recent_count: usize,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_capability_id() {
        let cap = CapabilityId::new("vr.headset");
        assert_eq!(cap.as_str(), "vr.headset");
        assert_eq!(cap.to_string(), "vr.headset");
    }

    #[test]
    fn test_world_metadata() {
        let metadata = WorldMetadata::new("A test world")
            .with_tag("social")
            .with_tag("relaxing")
            .with_category("hangout")
            .with_rating("Everyone");

        assert_eq!(metadata.description, "A test world");
        assert!(metadata.has_tag("social"));
        assert!(metadata.has_tag("SOCIAL")); // Case insensitive
        assert!(!metadata.has_tag("pvp"));
        assert_eq!(metadata.category, Some("hangout".to_string()));
    }

    #[test]
    fn test_world_server_info() {
        let info = WorldServerInfo::new("Test World", "ws://localhost:8080")
            .with_capacity(50)
            .with_population(25)
            .with_region("us-west");

        assert_eq!(info.name, "Test World");
        assert_eq!(info.capacity, 50);
        assert_eq!(info.population, 25);
        assert!(!info.is_full());
        assert!((info.fill_percentage() - 0.5).abs() < 0.001);

        let full_info = info.with_population(50);
        assert!(full_info.is_full());
    }

    #[test]
    fn test_world_config() {
        let config = WorldConfig::new("My World")
            .with_description("A great world")
            .with_max_players(200)
            .with_password("secret123");

        assert_eq!(config.name, "My World");
        assert_eq!(config.max_players, 200);
        assert!(!config.public);
        assert_eq!(config.password, Some("secret123".to_string()));
    }

    #[test]
    fn test_world_registry_basic() {
        let mut registry = WorldRegistry::new();

        let info = WorldServerInfo::new("Test", "ws://localhost:8080");
        let world_id = info.world_id;

        registry.add_direct(info);

        assert!(registry.get_world(&world_id).is_some());
        assert_eq!(registry.all_worlds().len(), 1);
    }

    #[test]
    fn test_world_registry_favorites() {
        let mut registry = WorldRegistry::new();

        let info = WorldServerInfo::new("Favorite World", "ws://localhost:8080");
        let world_id = info.world_id;

        registry.add_direct(info);
        registry.add_favorite(world_id);

        assert_eq!(registry.favorites().len(), 1);

        registry.remove_favorite(&world_id);
        assert_eq!(registry.favorites().len(), 0);
    }

    #[test]
    fn test_world_registry_recent() {
        let mut registry = WorldRegistry::new();

        for i in 0..25 {
            let info = WorldServerInfo::new(format!("World {}", i), "ws://localhost:8080");
            let world_id = info.world_id;
            registry.add_direct(info);
            registry.record_visit(world_id);
        }

        // Should only keep 20 recent
        assert_eq!(registry.recent.len(), 20);
    }

    #[test]
    fn test_world_registry_find() {
        let mut registry = WorldRegistry::new();

        let info1 = WorldServerInfo::new("Social Hub", "ws://localhost:8080")
            .with_population(10)
            .with_capacity(100)
            .with_metadata(WorldMetadata::new("").with_tag("social"));

        let info2 = WorldServerInfo::new("Game World", "ws://localhost:8081")
            .with_population(100)
            .with_capacity(100)
            .with_metadata(WorldMetadata::new("").with_tag("game"));

        registry.add_direct(info1);
        registry.add_direct(info2);

        let social_worlds = registry.find_by_tag("social");
        assert_eq!(social_worlds.len(), 1);

        let available = registry.find_available();
        assert_eq!(available.len(), 1); // Only social hub has space
    }

    #[test]
    fn test_registry_stats() {
        let mut registry = WorldRegistry::new();

        let info1 = WorldServerInfo::new("World 1", "ws://localhost:8080")
            .with_population(10)
            .with_capacity(100);

        let info2 = WorldServerInfo::new("World 2", "ws://localhost:8081")
            .with_population(50)
            .with_capacity(200);

        registry.add_direct(info1);
        registry.add_direct(info2);

        let stats = registry.stats();
        assert_eq!(stats.total_worlds, 2);
        assert_eq!(stats.online_worlds, 2);
        assert_eq!(stats.total_players, 60);
        assert_eq!(stats.total_capacity, 300);
    }
}
