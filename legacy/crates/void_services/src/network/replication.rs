//! Entity Replication
//!
//! Handles synchronization of entity state across network nodes.
//! Implements authority model, delta compression, and conflict resolution.

use std::collections::{HashMap, HashSet, VecDeque};
use std::time::Instant;

use serde::{Deserialize, Serialize};
use uuid::Uuid;

use super::consistency::ConsistencyLevel;

/// Unique identifier for an entity (same across all nodes)
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct EntityId(Uuid);

impl EntityId {
    /// Create a new random entity ID
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

    /// Convert to bytes for network transmission
    pub fn to_bytes(&self) -> [u8; 16] {
        *self.0.as_bytes()
    }

    /// Create from bytes
    pub fn from_bytes(bytes: [u8; 16]) -> Self {
        Self(Uuid::from_bytes(bytes))
    }
}

impl Default for EntityId {
    fn default() -> Self {
        Self::new()
    }
}

impl std::fmt::Display for EntityId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}

/// Component type identifier
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct ComponentTypeId(u64);

impl ComponentTypeId {
    /// Create a new component type ID
    pub fn new(id: u64) -> Self {
        Self(id)
    }

    /// Create from a type name (hashed)
    pub fn from_name(name: &str) -> Self {
        use std::collections::hash_map::DefaultHasher;
        use std::hash::{Hash, Hasher};

        let mut hasher = DefaultHasher::new();
        name.hash(&mut hasher);
        Self(hasher.finish())
    }

    /// Get the raw ID value
    pub fn id(&self) -> u64 {
        self.0
    }
}

/// Client identifier
#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct ClientId(String);

impl ClientId {
    /// Create a new client ID
    pub fn new(id: impl Into<String>) -> Self {
        Self(id.into())
    }

    /// Generate a random client ID
    pub fn generate() -> Self {
        Self(Uuid::new_v4().to_string())
    }

    /// Get the ID string
    pub fn as_str(&self) -> &str {
        &self.0
    }
}

impl std::fmt::Display for ClientId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}

/// Node identifier in distributed system
#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct NodeId(String);

impl NodeId {
    /// Create a new node ID
    pub fn new(id: impl Into<String>) -> Self {
        Self(id.into())
    }

    /// Get the ID string
    pub fn as_str(&self) -> &str {
        &self.0
    }
}

/// Authority model for entity ownership
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum Authority {
    /// Server is authoritative (NPCs, world objects)
    Server,

    /// Specific client owns this entity (player avatars)
    Client(ClientId),

    /// Distributed ownership with primary and replicas
    Distributed {
        /// Primary owner
        primary: NodeId,
        /// Replica nodes
        replicas: Vec<NodeId>,
    },
}

impl Authority {
    /// Check if the given client is authoritative
    pub fn is_client_authority(&self, client_id: &ClientId) -> bool {
        match self {
            Authority::Client(id) => id == client_id,
            _ => false,
        }
    }

    /// Check if server is authoritative
    pub fn is_server_authority(&self) -> bool {
        matches!(self, Authority::Server)
    }
}

impl Default for Authority {
    fn default() -> Self {
        Authority::Server
    }
}

/// Update to a single component
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ComponentUpdate {
    /// Full component replacement
    Set {
        /// Component type identifier
        type_id: ComponentTypeId,
        /// Serialized component data
        data: Vec<u8>,
    },

    /// Delta update for large components
    Delta {
        /// Component type identifier
        type_id: ComponentTypeId,
        /// Delta patch data
        delta: Vec<u8>,
        /// Base sequence this delta applies to
        base_sequence: u64,
    },

    /// Component removed
    Remove {
        /// Component type identifier
        type_id: ComponentTypeId,
    },
}

impl ComponentUpdate {
    /// Create a set update
    pub fn set(type_id: ComponentTypeId, data: Vec<u8>) -> Self {
        Self::Set { type_id, data }
    }

    /// Create a delta update
    pub fn delta(type_id: ComponentTypeId, delta: Vec<u8>, base_sequence: u64) -> Self {
        Self::Delta {
            type_id,
            delta,
            base_sequence,
        }
    }

    /// Create a remove update
    pub fn remove(type_id: ComponentTypeId) -> Self {
        Self::Remove { type_id }
    }

    /// Get the component type ID
    pub fn type_id(&self) -> ComponentTypeId {
        match self {
            Self::Set { type_id, .. } => *type_id,
            Self::Delta { type_id, .. } => *type_id,
            Self::Remove { type_id } => *type_id,
        }
    }

    /// Get the data size
    pub fn data_size(&self) -> usize {
        match self {
            Self::Set { data, .. } => data.len(),
            Self::Delta { delta, .. } => delta.len(),
            Self::Remove { .. } => 0,
        }
    }
}

/// Entity update message
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EntityUpdate {
    /// Entity being updated
    pub entity: EntityId,
    /// Sequence number for ordering
    pub sequence: u64,
    /// Component updates
    pub components: Vec<ComponentUpdate>,
    /// Timestamp (milliseconds since epoch)
    pub timestamp: u64,
    /// Authority of the sender
    pub authority: Authority,
    /// Consistency level for this update
    pub consistency: ConsistencyLevel,
}

impl EntityUpdate {
    /// Create a new entity update
    pub fn new(entity: EntityId, sequence: u64) -> Self {
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default()
            .as_millis() as u64;

        Self {
            entity,
            sequence,
            components: Vec::new(),
            timestamp,
            authority: Authority::Server,
            consistency: ConsistencyLevel::Eventual,
        }
    }

    /// Add a component update
    pub fn with_component(mut self, update: ComponentUpdate) -> Self {
        self.components.push(update);
        self
    }

    /// Set the authority
    pub fn with_authority(mut self, authority: Authority) -> Self {
        self.authority = authority;
        self
    }

    /// Set the consistency level
    pub fn with_consistency(mut self, consistency: ConsistencyLevel) -> Self {
        self.consistency = consistency;
        self
    }

    /// Check if this update is newer than another
    pub fn is_newer_than(&self, other: &EntityUpdate) -> bool {
        self.sequence > other.sequence
            || (self.sequence == other.sequence && self.timestamp > other.timestamp)
    }

    /// Get total data size
    pub fn data_size(&self) -> usize {
        self.components.iter().map(|c| c.data_size()).sum()
    }
}

/// Replicated entity state
#[derive(Debug, Clone)]
pub struct ReplicatedEntity {
    /// Entity ID
    pub id: EntityId,
    /// Current authority
    pub authority: Authority,
    /// Last known component data (type_id -> data)
    pub components: HashMap<ComponentTypeId, Vec<u8>>,
    /// Last update sequence number
    pub last_sequence: u64,
    /// Time of last update
    pub last_update: Instant,
    /// Consistency level for this entity
    pub consistency: ConsistencyLevel,
}

impl ReplicatedEntity {
    /// Create a new replicated entity
    pub fn new(id: EntityId, authority: Authority) -> Self {
        Self {
            id,
            authority,
            components: HashMap::new(),
            last_sequence: 0,
            last_update: Instant::now(),
            consistency: ConsistencyLevel::Eventual,
        }
    }

    /// Apply a component update
    pub fn apply_update(&mut self, update: &ComponentUpdate) {
        match update {
            ComponentUpdate::Set { type_id, data } => {
                self.components.insert(*type_id, data.clone());
            }
            ComponentUpdate::Delta { type_id, delta, .. } => {
                // For delta updates, we need to apply the patch to existing data
                // This is a simplified version - real implementation would use proper diffing
                if let Some(existing) = self.components.get_mut(type_id) {
                    // Apply delta (simplified: just replace for now)
                    *existing = delta.clone();
                } else {
                    self.components.insert(*type_id, delta.clone());
                }
            }
            ComponentUpdate::Remove { type_id } => {
                self.components.remove(type_id);
            }
        }
        self.last_update = Instant::now();
    }

    /// Get a component's data
    pub fn get_component(&self, type_id: ComponentTypeId) -> Option<&Vec<u8>> {
        self.components.get(&type_id)
    }
}

/// Entity replicator - manages entity synchronization
pub struct EntityReplicator {
    /// Our client ID
    client_id: ClientId,
    /// Entities we own (authoritative)
    owned: HashSet<EntityId>,
    /// Entities we're observing (replicas)
    replicas: HashMap<EntityId, ReplicatedEntity>,
    /// Pending updates to send
    outgoing: VecDeque<EntityUpdate>,
    /// Received updates to apply
    incoming: VecDeque<EntityUpdate>,
    /// Next sequence number
    sequence: u64,
    /// Per-component consistency overrides
    consistency_overrides: HashMap<ComponentTypeId, ConsistencyLevel>,
    /// Rate limiting: last update time per entity
    last_update_time: HashMap<EntityId, Instant>,
    /// Minimum update interval (for rate limiting)
    min_update_interval: std::time::Duration,
}

impl EntityReplicator {
    /// Create a new entity replicator
    pub fn new() -> Self {
        Self {
            client_id: ClientId::generate(),
            owned: HashSet::new(),
            replicas: HashMap::new(),
            outgoing: VecDeque::new(),
            incoming: VecDeque::new(),
            sequence: 0,
            consistency_overrides: HashMap::new(),
            last_update_time: HashMap::new(),
            min_update_interval: std::time::Duration::from_millis(16), // ~60 FPS
        }
    }

    /// Set the client ID
    pub fn set_client_id(&mut self, client_id: ClientId) {
        self.client_id = client_id;
    }

    /// Get the client ID
    pub fn client_id(&self) -> &ClientId {
        &self.client_id
    }

    /// Mark entity as owned by this client (we are authority)
    pub fn claim_authority(&mut self, entity: EntityId) {
        self.owned.insert(entity);
        log::debug!("Claimed authority for entity {}", entity);
    }

    /// Release authority over an entity
    pub fn release_authority(&mut self, entity: EntityId) {
        self.owned.remove(&entity);
        log::debug!("Released authority for entity {}", entity);
    }

    /// Check if we have authority over an entity
    pub fn has_authority(&self, entity: &EntityId) -> bool {
        self.owned.contains(entity)
    }

    /// Get all owned entities
    pub fn owned_entities(&self) -> &HashSet<EntityId> {
        &self.owned
    }

    /// Get all replica entities
    pub fn replicas(&self) -> &HashMap<EntityId, ReplicatedEntity> {
        &self.replicas
    }

    /// Get a specific replica
    pub fn get_replica(&self, entity: &EntityId) -> Option<&ReplicatedEntity> {
        self.replicas.get(entity)
    }

    /// Set consistency level for a component type
    pub fn set_component_consistency(
        &mut self,
        type_id: ComponentTypeId,
        level: ConsistencyLevel,
    ) {
        self.consistency_overrides.insert(type_id, level);
    }

    /// Get consistency level for a component type
    pub fn get_consistency(&self, type_id: ComponentTypeId) -> ConsistencyLevel {
        self.consistency_overrides
            .get(&type_id)
            .copied()
            .unwrap_or(ConsistencyLevel::Eventual)
    }

    /// Set minimum update interval for rate limiting
    pub fn set_min_update_interval(&mut self, interval: std::time::Duration) {
        self.min_update_interval = interval;
    }

    /// Get the next sequence number
    fn next_sequence(&mut self) -> u64 {
        self.sequence += 1;
        self.sequence
    }

    /// Check if an update should be rate-limited
    fn should_rate_limit(&self, entity: &EntityId) -> bool {
        if let Some(last_time) = self.last_update_time.get(entity) {
            last_time.elapsed() < self.min_update_interval
        } else {
            false
        }
    }

    /// Queue a component update for replication
    pub fn replicate_component(
        &mut self,
        entity: EntityId,
        type_id: ComponentTypeId,
        data: Vec<u8>,
    ) {
        // Can only replicate entities we own
        if !self.owned.contains(&entity) {
            log::warn!(
                "Cannot replicate component for entity {} - not owned",
                entity
            );
            return;
        }

        // Rate limiting
        if self.should_rate_limit(&entity) {
            return;
        }

        let sequence = self.next_sequence();
        let consistency = self.get_consistency(type_id);

        let update = EntityUpdate::new(entity, sequence)
            .with_component(ComponentUpdate::set(type_id, data))
            .with_authority(Authority::Client(self.client_id.clone()))
            .with_consistency(consistency);

        self.outgoing.push_back(update);
        self.last_update_time.insert(entity, Instant::now());
    }

    /// Queue multiple component updates for an entity
    pub fn replicate_components(
        &mut self,
        entity: EntityId,
        components: Vec<(ComponentTypeId, Vec<u8>)>,
    ) {
        if !self.owned.contains(&entity) {
            log::warn!(
                "Cannot replicate components for entity {} - not owned",
                entity
            );
            return;
        }

        if self.should_rate_limit(&entity) {
            return;
        }

        let sequence = self.next_sequence();

        let mut update = EntityUpdate::new(entity, sequence)
            .with_authority(Authority::Client(self.client_id.clone()));

        for (type_id, data) in components {
            update.components.push(ComponentUpdate::set(type_id, data));
        }

        self.outgoing.push_back(update);
        self.last_update_time.insert(entity, Instant::now());
    }

    /// Queue a component removal
    pub fn replicate_remove(&mut self, entity: EntityId, type_id: ComponentTypeId) {
        if !self.owned.contains(&entity) {
            return;
        }

        let sequence = self.next_sequence();

        let update = EntityUpdate::new(entity, sequence)
            .with_component(ComponentUpdate::remove(type_id))
            .with_authority(Authority::Client(self.client_id.clone()));

        self.outgoing.push_back(update);
    }

    /// Queue an incoming update
    pub fn receive_update(&mut self, update: EntityUpdate) {
        self.incoming.push_back(update);
    }

    /// Apply received updates
    pub fn apply_updates(&mut self) -> Vec<EntityUpdate> {
        let mut applied = Vec::new();

        while let Some(update) = self.incoming.pop_front() {
            // Skip updates for entities we own (we're authoritative)
            if self.owned.contains(&update.entity) {
                // Unless it's server authoritative
                if !matches!(update.authority, Authority::Server) {
                    continue;
                }
            }

            // Get or create replica
            let replica = self.replicas.entry(update.entity).or_insert_with(|| {
                ReplicatedEntity::new(update.entity, update.authority.clone())
            });

            // Check if update is stale
            if update.sequence <= replica.last_sequence {
                // For strong consistency, we might still need to apply
                if update.consistency != ConsistencyLevel::Strong {
                    continue;
                }
            }

            // Apply component updates
            for component_update in &update.components {
                replica.apply_update(component_update);
            }

            replica.last_sequence = update.sequence;
            replica.authority = update.authority.clone();
            applied.push(update);
        }

        applied
    }

    /// Take outgoing updates (drains the queue)
    pub fn take_outgoing(&mut self) -> Vec<EntityUpdate> {
        self.outgoing.drain(..).collect()
    }

    /// Check if there are pending outgoing updates
    pub fn has_outgoing(&self) -> bool {
        !self.outgoing.is_empty()
    }

    /// Get the number of pending outgoing updates
    pub fn outgoing_count(&self) -> usize {
        self.outgoing.len()
    }

    /// Remove a replica (entity despawned)
    pub fn remove_replica(&mut self, entity: &EntityId) {
        self.replicas.remove(entity);
        self.last_update_time.remove(entity);
    }

    /// Clear all replicas
    pub fn clear_replicas(&mut self) {
        self.replicas.clear();
        self.last_update_time.clear();
    }

    /// Get replication statistics
    pub fn stats(&self) -> ReplicationStats {
        ReplicationStats {
            owned_count: self.owned.len(),
            replica_count: self.replicas.len(),
            outgoing_count: self.outgoing.len(),
            incoming_count: self.incoming.len(),
            current_sequence: self.sequence,
        }
    }
}

impl Default for EntityReplicator {
    fn default() -> Self {
        Self::new()
    }
}

/// Replication statistics
#[derive(Debug, Clone)]
pub struct ReplicationStats {
    /// Number of owned entities
    pub owned_count: usize,
    /// Number of replica entities
    pub replica_count: usize,
    /// Number of pending outgoing updates
    pub outgoing_count: usize,
    /// Number of pending incoming updates
    pub incoming_count: usize,
    /// Current sequence number
    pub current_sequence: u64,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_entity_id() {
        let id1 = EntityId::new();
        let id2 = EntityId::new();
        assert_ne!(id1, id2);

        let bytes = id1.to_bytes();
        let restored = EntityId::from_bytes(bytes);
        assert_eq!(id1, restored);

        let id_str = id1.to_string();
        let parsed = EntityId::parse(&id_str).unwrap();
        assert_eq!(id1, parsed);
    }

    #[test]
    fn test_component_type_id() {
        let id1 = ComponentTypeId::from_name("Position");
        let id2 = ComponentTypeId::from_name("Position");
        assert_eq!(id1, id2);

        let id3 = ComponentTypeId::from_name("Velocity");
        assert_ne!(id1, id3);
    }

    #[test]
    fn test_authority() {
        let client_id = ClientId::new("client1");

        let server_auth = Authority::Server;
        assert!(server_auth.is_server_authority());
        assert!(!server_auth.is_client_authority(&client_id));

        let client_auth = Authority::Client(client_id.clone());
        assert!(!client_auth.is_server_authority());
        assert!(client_auth.is_client_authority(&client_id));

        let other_client = ClientId::new("other");
        assert!(!client_auth.is_client_authority(&other_client));
    }

    #[test]
    fn test_component_update() {
        let type_id = ComponentTypeId::from_name("Test");

        let set = ComponentUpdate::set(type_id, vec![1, 2, 3]);
        assert_eq!(set.type_id(), type_id);
        assert_eq!(set.data_size(), 3);

        let remove = ComponentUpdate::remove(type_id);
        assert_eq!(remove.type_id(), type_id);
        assert_eq!(remove.data_size(), 0);
    }

    #[test]
    fn test_entity_update() {
        let entity = EntityId::new();
        let type_id = ComponentTypeId::from_name("Position");

        let update = EntityUpdate::new(entity, 1)
            .with_component(ComponentUpdate::set(type_id, vec![1, 2, 3, 4]))
            .with_authority(Authority::Server)
            .with_consistency(ConsistencyLevel::Strong);

        assert_eq!(update.entity, entity);
        assert_eq!(update.sequence, 1);
        assert_eq!(update.components.len(), 1);
        assert_eq!(update.data_size(), 4);
    }

    #[test]
    fn test_replicated_entity() {
        let id = EntityId::new();
        let mut entity = ReplicatedEntity::new(id, Authority::Server);

        let type_id = ComponentTypeId::from_name("Position");
        let update = ComponentUpdate::set(type_id, vec![1, 2, 3]);
        entity.apply_update(&update);

        assert!(entity.get_component(type_id).is_some());
        assert_eq!(entity.get_component(type_id).unwrap(), &vec![1, 2, 3]);

        let remove = ComponentUpdate::remove(type_id);
        entity.apply_update(&remove);
        assert!(entity.get_component(type_id).is_none());
    }

    #[test]
    fn test_entity_replicator_authority() {
        let mut replicator = EntityReplicator::new();

        let entity = EntityId::new();
        assert!(!replicator.has_authority(&entity));

        replicator.claim_authority(entity);
        assert!(replicator.has_authority(&entity));

        replicator.release_authority(entity);
        assert!(!replicator.has_authority(&entity));
    }

    #[test]
    fn test_entity_replicator_replicate() {
        let mut replicator = EntityReplicator::new();
        replicator.set_min_update_interval(std::time::Duration::ZERO);

        let entity = EntityId::new();
        let type_id = ComponentTypeId::from_name("Position");

        // Can't replicate without authority
        replicator.replicate_component(entity, type_id, vec![1, 2, 3]);
        assert_eq!(replicator.outgoing_count(), 0);

        // Claim authority and replicate
        replicator.claim_authority(entity);
        replicator.replicate_component(entity, type_id, vec![1, 2, 3]);
        assert_eq!(replicator.outgoing_count(), 1);

        let updates = replicator.take_outgoing();
        assert_eq!(updates.len(), 1);
        assert_eq!(updates[0].entity, entity);
    }

    #[test]
    fn test_entity_replicator_apply_updates() {
        let mut replicator = EntityReplicator::new();

        let entity = EntityId::new();
        let type_id = ComponentTypeId::from_name("Position");

        let update = EntityUpdate::new(entity, 1)
            .with_component(ComponentUpdate::set(type_id, vec![1, 2, 3]))
            .with_authority(Authority::Server);

        replicator.receive_update(update);
        let applied = replicator.apply_updates();

        assert_eq!(applied.len(), 1);
        assert!(replicator.get_replica(&entity).is_some());

        let replica = replicator.get_replica(&entity).unwrap();
        assert_eq!(replica.get_component(type_id).unwrap(), &vec![1, 2, 3]);
    }

    #[test]
    fn test_entity_replicator_stats() {
        let mut replicator = EntityReplicator::new();
        replicator.set_min_update_interval(std::time::Duration::ZERO);

        let entity1 = EntityId::new();
        let entity2 = EntityId::new();

        replicator.claim_authority(entity1);
        replicator.replicate_component(entity1, ComponentTypeId::new(1), vec![1]);

        let update = EntityUpdate::new(entity2, 1).with_authority(Authority::Server);
        replicator.receive_update(update);
        replicator.apply_updates();

        let stats = replicator.stats();
        assert_eq!(stats.owned_count, 1);
        assert_eq!(stats.replica_count, 1);
        assert_eq!(stats.outgoing_count, 1);
    }
}
