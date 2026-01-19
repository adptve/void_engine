//! Graph - Visual script graph structure
//!
//! A graph connects nodes together to form a complete visual script.

use crate::node::{Node, NodeId, PinId, NodeRegistry, NodeResult, NodeContext};
use crate::value::Value;
use alloc::collections::BTreeMap;
use alloc::string::{String, ToString};
use alloc::vec::Vec;
use void_core::Id;

/// Unique identifier for a connection
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct ConnectionId(pub Id);

impl ConnectionId {
    pub fn new(id: u64) -> Self {
        Self(Id::from_bits(id))
    }
}

/// A connection between two pins
#[derive(Clone, Debug)]
pub struct Connection {
    /// Connection ID
    pub id: ConnectionId,
    /// Source node
    pub from_node: NodeId,
    /// Source pin
    pub from_pin: PinId,
    /// Target node
    pub to_node: NodeId,
    /// Target pin
    pub to_pin: PinId,
}

impl Connection {
    /// Create a new connection
    pub fn new(
        id: ConnectionId,
        from_node: NodeId,
        from_pin: PinId,
        to_node: NodeId,
        to_pin: PinId,
    ) -> Self {
        Self {
            id,
            from_node,
            from_pin,
            to_node,
            to_pin,
        }
    }
}

/// Graph variable
#[derive(Clone, Debug)]
pub struct Variable {
    /// Variable name
    pub name: String,
    /// Variable type
    pub value_type: crate::value::ValueType,
    /// Current value
    pub value: Value,
    /// Is this a local variable
    pub is_local: bool,
    /// Is this exposed to the editor
    pub is_exposed: bool,
}

impl Variable {
    /// Create a new variable
    pub fn new(name: &str, value_type: crate::value::ValueType) -> Self {
        Self {
            name: name.to_string(),
            value: value_type.default_value(),
            value_type,
            is_local: true,
            is_exposed: false,
        }
    }

    /// Set initial value
    pub fn with_value(mut self, value: Value) -> Self {
        self.value = value;
        self
    }

    /// Mark as exposed
    pub fn exposed(mut self) -> Self {
        self.is_exposed = true;
        self
    }
}

/// The visual script graph
#[derive(Clone, Debug)]
pub struct Graph {
    /// Graph name
    pub name: String,
    /// All nodes
    nodes: BTreeMap<NodeId, Node>,
    /// All connections
    connections: BTreeMap<ConnectionId, Connection>,
    /// Variables
    variables: BTreeMap<String, Variable>,
    /// Next node ID
    next_node_id: u64,
    /// Next connection ID
    next_connection_id: u64,
    /// Entry point nodes (events)
    entry_points: Vec<NodeId>,
    /// Graph metadata
    pub metadata: BTreeMap<String, String>,
}

impl Graph {
    /// Create a new empty graph
    pub fn new(name: &str) -> Self {
        Self {
            name: name.to_string(),
            nodes: BTreeMap::new(),
            connections: BTreeMap::new(),
            variables: BTreeMap::new(),
            next_node_id: 1,
            next_connection_id: 1,
            entry_points: Vec::new(),
            metadata: BTreeMap::new(),
        }
    }

    /// Add a node to the graph
    pub fn add_node(&mut self, type_id: &str) -> NodeId {
        let id = NodeId::new(self.next_node_id);
        self.next_node_id += 1;

        let node = Node::new(id, type_id);
        self.nodes.insert(id, node);
        id
    }

    /// Add a node with a specific ID
    pub fn add_node_with_id(&mut self, id: NodeId, type_id: &str) {
        let node = Node::new(id, type_id);
        self.nodes.insert(id, node);
    }

    /// Remove a node and all its connections
    pub fn remove_node(&mut self, id: NodeId) -> Option<Node> {
        // Remove all connections involving this node
        let to_remove: Vec<_> = self.connections
            .iter()
            .filter(|(_, c)| c.from_node == id || c.to_node == id)
            .map(|(id, _)| *id)
            .collect();

        for conn_id in to_remove {
            self.connections.remove(&conn_id);
        }

        // Remove from entry points
        self.entry_points.retain(|&e| e != id);

        self.nodes.remove(&id)
    }

    /// Get a node
    pub fn get_node(&self, id: NodeId) -> Option<&Node> {
        self.nodes.get(&id)
    }

    /// Get a mutable node
    pub fn get_node_mut(&mut self, id: NodeId) -> Option<&mut Node> {
        self.nodes.get_mut(&id)
    }

    /// Connect two pins
    pub fn connect(
        &mut self,
        from_node: NodeId,
        from_pin: PinId,
        to_node: NodeId,
        to_pin: PinId,
    ) -> Result<ConnectionId, String> {
        // Validate nodes exist
        if !self.nodes.contains_key(&from_node) {
            return Err("Source node does not exist".into());
        }
        if !self.nodes.contains_key(&to_node) {
            return Err("Target node does not exist".into());
        }

        // Check for existing connection to the same input pin
        if self.connections.values().any(|c| c.to_node == to_node && c.to_pin == to_pin) {
            // For data pins, replace the connection
            // For exec pins, we might want to allow multiple
            let to_remove: Vec<_> = self.connections
                .iter()
                .filter(|(_, c)| c.to_node == to_node && c.to_pin == to_pin)
                .map(|(id, _)| *id)
                .collect();

            for id in to_remove {
                self.connections.remove(&id);
            }
        }

        let id = ConnectionId::new(self.next_connection_id);
        self.next_connection_id += 1;

        let connection = Connection::new(id, from_node, from_pin, to_node, to_pin);
        self.connections.insert(id, connection);

        Ok(id)
    }

    /// Disconnect a connection
    pub fn disconnect(&mut self, id: ConnectionId) -> Option<Connection> {
        self.connections.remove(&id)
    }

    /// Disconnect all connections from a pin
    pub fn disconnect_pin(&mut self, node: NodeId, pin: PinId) {
        let to_remove: Vec<_> = self.connections
            .iter()
            .filter(|(_, c)| {
                (c.from_node == node && c.from_pin == pin)
                    || (c.to_node == node && c.to_pin == pin)
            })
            .map(|(id, _)| *id)
            .collect();

        for id in to_remove {
            self.connections.remove(&id);
        }
    }

    /// Get connections from a pin
    pub fn get_connections_from(&self, node: NodeId, pin: PinId) -> Vec<&Connection> {
        self.connections
            .values()
            .filter(|c| c.from_node == node && c.from_pin == pin)
            .collect()
    }

    /// Get the connection to a pin (only one allowed for data pins)
    pub fn get_connection_to(&self, node: NodeId, pin: PinId) -> Option<&Connection> {
        self.connections
            .values()
            .find(|c| c.to_node == node && c.to_pin == pin)
    }

    /// Add a variable
    pub fn add_variable(&mut self, variable: Variable) {
        self.variables.insert(variable.name.clone(), variable);
    }

    /// Remove a variable
    pub fn remove_variable(&mut self, name: &str) -> Option<Variable> {
        self.variables.remove(name)
    }

    /// Get a variable
    pub fn get_variable(&self, name: &str) -> Option<&Variable> {
        self.variables.get(name)
    }

    /// Get a mutable variable
    pub fn get_variable_mut(&mut self, name: &str) -> Option<&mut Variable> {
        self.variables.get_mut(name)
    }

    /// Set a variable value
    pub fn set_variable(&mut self, name: &str, value: Value) -> bool {
        if let Some(var) = self.variables.get_mut(name) {
            var.value = value;
            true
        } else {
            false
        }
    }

    /// Mark a node as an entry point
    pub fn add_entry_point(&mut self, node: NodeId) {
        if !self.entry_points.contains(&node) {
            self.entry_points.push(node);
        }
    }

    /// Get entry points
    pub fn entry_points(&self) -> &[NodeId] {
        &self.entry_points
    }

    /// Get all nodes
    pub fn nodes(&self) -> impl Iterator<Item = &Node> {
        self.nodes.values()
    }

    /// Get all connections
    pub fn connections(&self) -> impl Iterator<Item = &Connection> {
        self.connections.values()
    }

    /// Get all variables
    pub fn variables(&self) -> impl Iterator<Item = &Variable> {
        self.variables.values()
    }

    /// Get node count
    pub fn node_count(&self) -> usize {
        self.nodes.len()
    }

    /// Get connection count
    pub fn connection_count(&self) -> usize {
        self.connections.len()
    }

    /// Get variable count
    pub fn variable_count(&self) -> usize {
        self.variables.len()
    }

    /// Validate the graph
    pub fn validate(&self, registry: &NodeRegistry) -> Vec<String> {
        let mut errors = Vec::new();

        // Check all nodes have valid definitions
        for node in self.nodes.values() {
            if registry.get_definition(&node.type_id).is_none() {
                errors.push(format!("Node {:?} has unknown type '{}'", node.id, node.type_id));
            }
        }

        // Check all connections are valid
        for conn in self.connections.values() {
            if !self.nodes.contains_key(&conn.from_node) {
                errors.push(format!("Connection {:?} has invalid source node", conn.id));
            }
            if !self.nodes.contains_key(&conn.to_node) {
                errors.push(format!("Connection {:?} has invalid target node", conn.id));
            }
        }

        // Check entry points exist
        for &entry in &self.entry_points {
            if !self.nodes.contains_key(&entry) {
                errors.push(format!("Entry point {:?} does not exist", entry));
            }
        }

        errors
    }

    /// Clear the graph
    pub fn clear(&mut self) {
        self.nodes.clear();
        self.connections.clear();
        self.entry_points.clear();
        self.next_node_id = 1;
        self.next_connection_id = 1;
    }
}

impl Default for Graph {
    fn default() -> Self {
        Self::new("Untitled")
    }
}

/// Execution context for running a graph
pub struct GraphExecutor<'a> {
    /// The graph being executed
    graph: &'a Graph,
    /// Node registry
    registry: &'a NodeRegistry,
    /// Variable values
    variables: BTreeMap<String, Value>,
    /// Node output cache
    node_outputs: BTreeMap<NodeId, BTreeMap<String, Value>>,
    /// Current execution path
    execution_stack: Vec<(NodeId, String)>,
    /// Delta time
    delta_time: f32,
}

impl<'a> GraphExecutor<'a> {
    /// Create a new executor
    pub fn new(graph: &'a Graph, registry: &'a NodeRegistry) -> Self {
        let variables = graph.variables()
            .map(|v| (v.name.clone(), v.value.clone()))
            .collect();

        Self {
            graph,
            registry,
            variables,
            node_outputs: BTreeMap::new(),
            execution_stack: Vec::new(),
            delta_time: 0.0,
        }
    }

    /// Set delta time
    pub fn set_delta_time(&mut self, dt: f32) {
        self.delta_time = dt;
    }

    /// Execute from an entry point
    pub fn execute(&mut self, entry_point: NodeId) -> Result<(), String> {
        self.execution_stack.clear();
        self.node_outputs.clear();

        self.execute_node(entry_point, "exec_in")
    }

    /// Execute a single node
    fn execute_node(&mut self, node_id: NodeId, _exec_pin: &str) -> Result<(), String> {
        // Prevent infinite loops
        if self.execution_stack.len() > 10000 {
            return Err("Execution stack overflow".into());
        }

        let node = self.graph.get_node(node_id)
            .ok_or_else(|| format!("Node {:?} not found", node_id))?;

        let executor = self.registry.get_executor(&node.type_id)
            .ok_or_else(|| format!("No executor for node type '{}'", node.type_id))?;

        // Gather inputs
        let inputs = self.gather_inputs(node_id)?;

        // Execute
        let mut outputs = BTreeMap::new();
        let mut context = NodeContext {
            inputs: &inputs,
            outputs: &mut outputs,
            node,
            delta_time: self.delta_time,
        };

        let result = executor.execute(&mut context);

        // Store outputs
        self.node_outputs.insert(node_id, outputs);

        // Handle result
        match result {
            NodeResult::Continue => {
                // Follow default exec output
                self.follow_exec(node_id, "exec_out")?;
            }
            NodeResult::Branch(output_name) => {
                // Follow specific exec output
                self.follow_exec(node_id, &output_name)?;
            }
            NodeResult::Error(msg) => {
                return Err(format!("Node {:?} error: {}", node_id, msg));
            }
            NodeResult::Done => {
                // Stop execution
            }
        }

        Ok(())
    }

    /// Gather input values for a node
    fn gather_inputs(&mut self, node_id: NodeId) -> Result<BTreeMap<String, Value>, String> {
        let node = self.graph.get_node(node_id).unwrap();
        let def = self.registry.get_definition(&node.type_id);

        let mut inputs = BTreeMap::new();

        if let Some(def) = def {
            for input_pin in &def.inputs {
                if input_pin.is_exec {
                    continue;
                }

                // Check for connection
                if let Some(conn) = self.graph.get_connection_to(node_id, input_pin.id) {
                    // Get value from connected node's output
                    let value = self.evaluate_output(conn.from_node, conn.from_pin)?;
                    inputs.insert(input_pin.name.clone(), value);
                } else if let Some(value) = node.get_pin_value(input_pin.id) {
                    // Use node's stored value
                    inputs.insert(input_pin.name.clone(), value.clone());
                } else {
                    // Use default
                    inputs.insert(input_pin.name.clone(), input_pin.default_value.clone());
                }
            }
        }

        Ok(inputs)
    }

    /// Evaluate an output pin's value
    fn evaluate_output(&mut self, node_id: NodeId, _pin_id: PinId) -> Result<Value, String> {
        // Check cache
        if let Some(outputs) = self.node_outputs.get(&node_id) {
            // Find the output by pin ID
            // For now, return first output (would need proper pin name lookup)
            if let Some(value) = outputs.values().next() {
                return Ok(value.clone());
            }
        }

        // Need to execute the node first (for pure functions)
        let node = self.graph.get_node(node_id)
            .ok_or_else(|| format!("Node {:?} not found", node_id))?;

        let def = self.registry.get_definition(&node.type_id)
            .ok_or_else(|| format!("Unknown node type '{}'", node.type_id))?;

        if def.is_pure {
            // Execute pure nodes on demand
            let inputs = self.gather_inputs(node_id)?;
            let executor = self.registry.get_executor(&node.type_id).unwrap();

            let mut outputs = BTreeMap::new();
            let mut context = NodeContext {
                inputs: &inputs,
                outputs: &mut outputs,
                node,
                delta_time: self.delta_time,
            };

            let _ = executor.execute(&mut context);
            self.node_outputs.insert(node_id, outputs.clone());

            if let Some(value) = outputs.values().next() {
                return Ok(value.clone());
            }
        }

        Ok(Value::None)
    }

    /// Follow an execution connection
    fn follow_exec(&mut self, from_node: NodeId, from_pin_name: &str) -> Result<(), String> {
        let from_pin = PinId::from_name(from_pin_name);

        let connections = self.graph.get_connections_from(from_node, from_pin);
        for conn in connections {
            // Execute with the target pin (exec pin name is derived from connection)
            self.execute_node(conn.to_node, "exec")?;
        }

        Ok(())
    }

    /// Get a variable value
    pub fn get_variable(&self, name: &str) -> Option<&Value> {
        self.variables.get(name)
    }

    /// Set a variable value
    pub fn set_variable(&mut self, name: &str, value: Value) {
        self.variables.insert(name.to_string(), value);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_graph_creation() {
        let mut graph = Graph::new("Test");

        let node1 = graph.add_node("event.begin_play");
        let node2 = graph.add_node("debug.print");

        assert_eq!(graph.node_count(), 2);

        let pin1 = PinId::from_name("exec_out");
        let pin2 = PinId::from_name("exec_in");

        let conn = graph.connect(node1, pin1, node2, pin2);
        assert!(conn.is_ok());
        assert_eq!(graph.connection_count(), 1);
    }

    #[test]
    fn test_graph_variables() {
        let mut graph = Graph::new("Test");

        graph.add_variable(Variable::new("health", crate::value::ValueType::Float)
            .with_value(Value::Float(100.0)));

        assert_eq!(graph.variable_count(), 1);
        assert!(graph.get_variable("health").is_some());
    }
}
