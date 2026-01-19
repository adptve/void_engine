//! Node - Visual scripting nodes
//!
//! Nodes are the building blocks of visual scripts.
//! Each node has input pins, output pins, and logic.

use crate::value::{Value, ValueType};
use alloc::boxed::Box;
use alloc::string::String;
use alloc::vec::Vec;
use alloc::collections::BTreeMap;
use core::any::TypeId;
use void_core::Id;

/// Unique identifier for a node
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct NodeId(pub Id);

impl NodeId {
    pub fn new(id: u64) -> Self {
        Self(Id::from_bits(id))
    }

    pub fn from_name(name: &str) -> Self {
        Self(Id::from_name(name))
    }

    pub fn id(&self) -> Id {
        self.0
    }
}

/// Unique identifier for a pin
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct PinId(pub Id);

impl PinId {
    pub fn new(id: u64) -> Self {
        Self(Id::from_bits(id))
    }

    pub fn from_name(name: &str) -> Self {
        Self(Id::from_name(name))
    }
}

/// Pin direction
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PinDirection {
    Input,
    Output,
}

/// Pin definition
#[derive(Clone, Debug)]
pub struct PinDef {
    /// Pin ID
    pub id: PinId,
    /// Pin name
    pub name: String,
    /// Pin direction
    pub direction: PinDirection,
    /// Value type
    pub value_type: ValueType,
    /// Default value
    pub default_value: Value,
    /// Is the pin required
    pub required: bool,
    /// Is this an execution pin
    pub is_exec: bool,
}

impl PinDef {
    /// Create a new input pin
    pub fn input(name: &str, value_type: ValueType) -> Self {
        Self {
            id: PinId::from_name(name),
            name: name.to_string(),
            direction: PinDirection::Input,
            default_value: value_type.default_value(),
            value_type,
            required: false,
            is_exec: false,
        }
    }

    /// Create a new output pin
    pub fn output(name: &str, value_type: ValueType) -> Self {
        Self {
            id: PinId::from_name(name),
            name: name.to_string(),
            direction: PinDirection::Output,
            default_value: value_type.default_value(),
            value_type,
            required: false,
            is_exec: false,
        }
    }

    /// Create an execution input pin
    pub fn exec_input(name: &str) -> Self {
        Self {
            id: PinId::from_name(name),
            name: name.to_string(),
            direction: PinDirection::Input,
            value_type: ValueType::Exec,
            default_value: Value::None,
            required: true,
            is_exec: true,
        }
    }

    /// Create an execution output pin
    pub fn exec_output(name: &str) -> Self {
        Self {
            id: PinId::from_name(name),
            name: name.to_string(),
            direction: PinDirection::Output,
            value_type: ValueType::Exec,
            default_value: Value::None,
            required: false,
            is_exec: true,
        }
    }

    /// Set as required
    pub fn required(mut self) -> Self {
        self.required = true;
        self
    }

    /// Set default value
    pub fn with_default(mut self, value: Value) -> Self {
        self.default_value = value;
        self
    }
}

/// Node category for organization
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum NodeCategory {
    /// Event nodes (entry points)
    Event,
    /// Flow control (branch, loop, sequence)
    FlowControl,
    /// Math operations
    Math,
    /// String operations
    String,
    /// Logic operations (and, or, not)
    Logic,
    /// Variable operations (get, set)
    Variable,
    /// Function calls
    Function,
    /// Entity operations
    Entity,
    /// Transform operations
    Transform,
    /// Physics operations
    Physics,
    /// Audio operations
    Audio,
    /// Input handling
    Input,
    /// Custom category
    Custom(String),
}

impl Default for NodeCategory {
    fn default() -> Self {
        Self::Function
    }
}

/// Node definition - describes a type of node
#[derive(Clone, Debug)]
pub struct NodeDef {
    /// Unique type ID for this node type
    pub type_id: String,
    /// Display name
    pub name: String,
    /// Description
    pub description: String,
    /// Category
    pub category: NodeCategory,
    /// Input pins
    pub inputs: Vec<PinDef>,
    /// Output pins
    pub outputs: Vec<PinDef>,
    /// Is this a pure function (no side effects)
    pub is_pure: bool,
    /// Is this an event node
    pub is_event: bool,
    /// Custom color
    pub color: Option<[f32; 3]>,
}

impl NodeDef {
    /// Create a new node definition
    pub fn new(type_id: &str, name: &str) -> Self {
        Self {
            type_id: type_id.to_string(),
            name: name.to_string(),
            description: String::new(),
            category: NodeCategory::default(),
            inputs: Vec::new(),
            outputs: Vec::new(),
            is_pure: false,
            is_event: false,
            color: None,
        }
    }

    /// Set description
    pub fn description(mut self, desc: &str) -> Self {
        self.description = desc.to_string();
        self
    }

    /// Set category
    pub fn category(mut self, cat: NodeCategory) -> Self {
        self.category = cat;
        self
    }

    /// Add an input pin
    pub fn input(mut self, pin: PinDef) -> Self {
        self.inputs.push(pin);
        self
    }

    /// Add an output pin
    pub fn output(mut self, pin: PinDef) -> Self {
        self.outputs.push(pin);
        self
    }

    /// Mark as pure function
    pub fn pure(mut self) -> Self {
        self.is_pure = true;
        self
    }

    /// Mark as event node
    pub fn event(mut self) -> Self {
        self.is_event = true;
        self.category = NodeCategory::Event;
        self
    }

    /// Set custom color
    pub fn color(mut self, r: f32, g: f32, b: f32) -> Self {
        self.color = Some([r, g, b]);
        self
    }

    /// Add execution flow pins
    pub fn with_exec_flow(mut self) -> Self {
        self.inputs.insert(0, PinDef::exec_input("exec_in"));
        self.outputs.insert(0, PinDef::exec_output("exec_out"));
        self
    }

    /// Get input pin by name
    pub fn get_input(&self, name: &str) -> Option<&PinDef> {
        self.inputs.iter().find(|p| p.name == name)
    }

    /// Get output pin by name
    pub fn get_output(&self, name: &str) -> Option<&PinDef> {
        self.outputs.iter().find(|p| p.name == name)
    }
}

/// Node instance in a graph
#[derive(Clone, Debug)]
pub struct Node {
    /// Unique instance ID
    pub id: NodeId,
    /// Node definition type ID
    pub type_id: String,
    /// Position in the graph editor
    pub position: [f32; 2],
    /// Pin values (for inputs with no connections)
    pub pin_values: BTreeMap<PinId, Value>,
    /// Custom metadata
    pub metadata: BTreeMap<String, String>,
}

impl Node {
    /// Create a new node instance
    pub fn new(id: NodeId, type_id: &str) -> Self {
        Self {
            id,
            type_id: type_id.to_string(),
            position: [0.0, 0.0],
            pin_values: BTreeMap::new(),
            metadata: BTreeMap::new(),
        }
    }

    /// Set position
    pub fn at(mut self, x: f32, y: f32) -> Self {
        self.position = [x, y];
        self
    }

    /// Set a pin value
    pub fn set_pin_value(&mut self, pin: PinId, value: Value) {
        self.pin_values.insert(pin, value);
    }

    /// Get a pin value
    pub fn get_pin_value(&self, pin: PinId) -> Option<&Value> {
        self.pin_values.get(&pin)
    }

    /// Set metadata
    pub fn set_metadata(&mut self, key: &str, value: &str) {
        self.metadata.insert(key.to_string(), value.to_string());
    }

    /// Get metadata
    pub fn get_metadata(&self, key: &str) -> Option<&str> {
        self.metadata.get(key).map(|s| s.as_str())
    }
}

/// Trait for node execution
pub trait NodeExecutor: Send + Sync {
    /// Get the node type ID
    fn type_id(&self) -> &str;

    /// Execute the node
    fn execute(&self, context: &mut NodeContext) -> NodeResult;
}

/// Context for node execution
pub struct NodeContext<'a> {
    /// Input values
    pub inputs: &'a BTreeMap<String, Value>,
    /// Output values (to be filled)
    pub outputs: &'a mut BTreeMap<String, Value>,
    /// Node instance data
    pub node: &'a Node,
    /// Delta time
    pub delta_time: f32,
}

impl<'a> NodeContext<'a> {
    /// Get an input value
    pub fn get_input(&self, name: &str) -> Option<&Value> {
        self.inputs.get(name)
    }

    /// Get input as specific type
    pub fn get_input_bool(&self, name: &str) -> Option<bool> {
        self.inputs.get(name).and_then(|v| v.as_bool())
    }

    pub fn get_input_int(&self, name: &str) -> Option<i32> {
        self.inputs.get(name).and_then(|v| v.as_int())
    }

    pub fn get_input_float(&self, name: &str) -> Option<f32> {
        self.inputs.get(name).and_then(|v| v.as_float())
    }

    pub fn get_input_string(&self, name: &str) -> Option<&str> {
        self.inputs.get(name).and_then(|v| v.as_string())
    }

    pub fn get_input_vec3(&self, name: &str) -> Option<[f32; 3]> {
        self.inputs.get(name).and_then(|v| v.as_vec3())
    }

    /// Set an output value
    pub fn set_output(&mut self, name: &str, value: Value) {
        self.outputs.insert(name.to_string(), value);
    }
}

/// Result of node execution
#[derive(Clone, Debug)]
pub enum NodeResult {
    /// Continue to the default output
    Continue,
    /// Branch to a specific output
    Branch(String),
    /// Error occurred
    Error(String),
    /// Node completed (for events)
    Done,
}

/// Node registry
pub struct NodeRegistry {
    /// Node definitions
    definitions: BTreeMap<String, NodeDef>,
    /// Node executors
    executors: BTreeMap<String, Box<dyn NodeExecutor>>,
}

impl NodeRegistry {
    /// Create a new registry
    pub fn new() -> Self {
        Self {
            definitions: BTreeMap::new(),
            executors: BTreeMap::new(),
        }
    }

    /// Register a node type
    pub fn register(&mut self, def: NodeDef, executor: Box<dyn NodeExecutor>) {
        let type_id = def.type_id.clone();
        self.definitions.insert(type_id.clone(), def);
        self.executors.insert(type_id, executor);
    }

    /// Get a node definition
    pub fn get_definition(&self, type_id: &str) -> Option<&NodeDef> {
        self.definitions.get(type_id)
    }

    /// Get a node executor
    pub fn get_executor(&self, type_id: &str) -> Option<&dyn NodeExecutor> {
        self.executors.get(type_id).map(|e| e.as_ref())
    }

    /// Get all definitions
    pub fn definitions(&self) -> impl Iterator<Item = &NodeDef> {
        self.definitions.values()
    }

    /// Get definitions by category
    pub fn definitions_by_category(&self, category: &NodeCategory) -> Vec<&NodeDef> {
        self.definitions
            .values()
            .filter(|d| &d.category == category)
            .collect()
    }
}

impl Default for NodeRegistry {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_node_definition() {
        let def = NodeDef::new("math.add", "Add")
            .description("Adds two numbers")
            .category(NodeCategory::Math)
            .input(PinDef::input("a", ValueType::Float))
            .input(PinDef::input("b", ValueType::Float))
            .output(PinDef::output("result", ValueType::Float))
            .pure();

        assert_eq!(def.name, "Add");
        assert_eq!(def.inputs.len(), 2);
        assert_eq!(def.outputs.len(), 1);
        assert!(def.is_pure);
    }

    #[test]
    fn test_pin_definition() {
        let input = PinDef::input("value", ValueType::Int)
            .required()
            .with_default(Value::Int(42));

        assert_eq!(input.name, "value");
        assert!(input.required);
        assert_eq!(input.default_value, Value::Int(42));
    }

    #[test]
    fn test_exec_flow() {
        let def = NodeDef::new("print", "Print")
            .input(PinDef::input("text", ValueType::String))
            .with_exec_flow();

        assert_eq!(def.inputs.len(), 2); // exec_in + text
        assert_eq!(def.outputs.len(), 1); // exec_out
        assert!(def.inputs[0].is_exec);
    }
}
