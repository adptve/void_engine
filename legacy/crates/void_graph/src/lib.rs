//! # void_graph - Visual Scripting System
//!
//! A Blueprint-compatible visual scripting system with:
//! - Node-based programming
//! - Event-driven execution
//! - Flow control (branch, loop, sequence)
//! - Variables and data flow
//! - Hot-reloadable graphs
//!
//! ## Compatibility with Unreal Blueprints
//!
//! The system uses similar concepts to Unreal Blueprints:
//! - **Execution Pins**: White pins for control flow
//! - **Data Pins**: Colored pins for data flow
//! - **Pure Functions**: Nodes without execution pins (evaluated on demand)
//! - **Events**: Entry points for graph execution
//! - **Variables**: Get/Set nodes for state
//!
//! ## Example
//!
//! ```ignore
//! use void_graph::prelude::*;
//!
//! // Create a graph
//! let mut graph = Graph::new("PlayerController");
//!
//! // Add variables
//! graph.add_variable(Variable::new("health", ValueType::Float)
//!     .with_value(Value::Float(100.0))
//!     .exposed());
//!
//! // Add nodes
//! let event_begin = graph.add_node("event.begin_play");
//! let print_node = graph.add_node("debug.print");
//!
//! // Connect nodes
//! graph.connect(
//!     event_begin, PinId::from_name("exec_out"),
//!     print_node, PinId::from_name("exec_in"),
//! )?;
//!
//! // Execute
//! let registry = create_default_registry();
//! let mut executor = GraphExecutor::new(&graph, &registry);
//! executor.execute(event_begin)?;
//! ```

#![cfg_attr(not(feature = "std"), no_std)]

extern crate alloc;

pub mod value;
pub mod node;
pub mod graph;

// Re-exports
pub use value::{Value, ValueType, CustomValue};
pub use node::{
    Node, NodeId, PinId, PinDef, PinDirection, NodeDef, NodeCategory,
    NodeExecutor, NodeContext, NodeResult, NodeRegistry,
};
pub use graph::{Graph, Connection, ConnectionId, Variable, GraphExecutor};

/// Prelude - commonly used types
pub mod prelude {
    pub use crate::value::{Value, ValueType};
    pub use crate::node::{
        Node, NodeId, PinId, PinDef, NodeDef, NodeCategory,
        NodeExecutor, NodeContext, NodeResult, NodeRegistry,
    };
    pub use crate::graph::{Graph, Connection, Variable, GraphExecutor};
}

/// Built-in node implementations
pub mod builtin {
    use super::*;
    use alloc::boxed::Box;
    use alloc::string::ToString;

    // ========== Math Nodes ==========

    pub struct AddNode;
    impl NodeExecutor for AddNode {
        fn type_id(&self) -> &str { "math.add" }
        fn execute(&self, ctx: &mut NodeContext) -> NodeResult {
            let a = ctx.get_input_float("a").unwrap_or(0.0);
            let b = ctx.get_input_float("b").unwrap_or(0.0);
            ctx.set_output("result", Value::Float(a + b));
            NodeResult::Continue
        }
    }

    pub struct SubtractNode;
    impl NodeExecutor for SubtractNode {
        fn type_id(&self) -> &str { "math.subtract" }
        fn execute(&self, ctx: &mut NodeContext) -> NodeResult {
            let a = ctx.get_input_float("a").unwrap_or(0.0);
            let b = ctx.get_input_float("b").unwrap_or(0.0);
            ctx.set_output("result", Value::Float(a - b));
            NodeResult::Continue
        }
    }

    pub struct MultiplyNode;
    impl NodeExecutor for MultiplyNode {
        fn type_id(&self) -> &str { "math.multiply" }
        fn execute(&self, ctx: &mut NodeContext) -> NodeResult {
            let a = ctx.get_input_float("a").unwrap_or(0.0);
            let b = ctx.get_input_float("b").unwrap_or(0.0);
            ctx.set_output("result", Value::Float(a * b));
            NodeResult::Continue
        }
    }

    pub struct DivideNode;
    impl NodeExecutor for DivideNode {
        fn type_id(&self) -> &str { "math.divide" }
        fn execute(&self, ctx: &mut NodeContext) -> NodeResult {
            let a = ctx.get_input_float("a").unwrap_or(0.0);
            let b = ctx.get_input_float("b").unwrap_or(1.0);
            if b.abs() < f32::EPSILON {
                ctx.set_output("result", Value::Float(0.0));
            } else {
                ctx.set_output("result", Value::Float(a / b));
            }
            NodeResult::Continue
        }
    }

    // ========== Logic Nodes ==========

    pub struct BranchNode;
    impl NodeExecutor for BranchNode {
        fn type_id(&self) -> &str { "flow.branch" }
        fn execute(&self, ctx: &mut NodeContext) -> NodeResult {
            let condition = ctx.get_input_bool("condition").unwrap_or(false);
            if condition {
                NodeResult::Branch("true".to_string())
            } else {
                NodeResult::Branch("false".to_string())
            }
        }
    }

    pub struct AndNode;
    impl NodeExecutor for AndNode {
        fn type_id(&self) -> &str { "logic.and" }
        fn execute(&self, ctx: &mut NodeContext) -> NodeResult {
            let a = ctx.get_input_bool("a").unwrap_or(false);
            let b = ctx.get_input_bool("b").unwrap_or(false);
            ctx.set_output("result", Value::Bool(a && b));
            NodeResult::Continue
        }
    }

    pub struct OrNode;
    impl NodeExecutor for OrNode {
        fn type_id(&self) -> &str { "logic.or" }
        fn execute(&self, ctx: &mut NodeContext) -> NodeResult {
            let a = ctx.get_input_bool("a").unwrap_or(false);
            let b = ctx.get_input_bool("b").unwrap_or(false);
            ctx.set_output("result", Value::Bool(a || b));
            NodeResult::Continue
        }
    }

    pub struct NotNode;
    impl NodeExecutor for NotNode {
        fn type_id(&self) -> &str { "logic.not" }
        fn execute(&self, ctx: &mut NodeContext) -> NodeResult {
            let value = ctx.get_input_bool("value").unwrap_or(false);
            ctx.set_output("result", Value::Bool(!value));
            NodeResult::Continue
        }
    }

    // ========== Comparison Nodes ==========

    pub struct EqualsNode;
    impl NodeExecutor for EqualsNode {
        fn type_id(&self) -> &str { "compare.equals" }
        fn execute(&self, ctx: &mut NodeContext) -> NodeResult {
            let a = ctx.get_input_float("a").unwrap_or(0.0);
            let b = ctx.get_input_float("b").unwrap_or(0.0);
            ctx.set_output("result", Value::Bool((a - b).abs() < f32::EPSILON));
            NodeResult::Continue
        }
    }

    pub struct LessThanNode;
    impl NodeExecutor for LessThanNode {
        fn type_id(&self) -> &str { "compare.less_than" }
        fn execute(&self, ctx: &mut NodeContext) -> NodeResult {
            let a = ctx.get_input_float("a").unwrap_or(0.0);
            let b = ctx.get_input_float("b").unwrap_or(0.0);
            ctx.set_output("result", Value::Bool(a < b));
            NodeResult::Continue
        }
    }

    pub struct GreaterThanNode;
    impl NodeExecutor for GreaterThanNode {
        fn type_id(&self) -> &str { "compare.greater_than" }
        fn execute(&self, ctx: &mut NodeContext) -> NodeResult {
            let a = ctx.get_input_float("a").unwrap_or(0.0);
            let b = ctx.get_input_float("b").unwrap_or(0.0);
            ctx.set_output("result", Value::Bool(a > b));
            NodeResult::Continue
        }
    }

    // ========== Event Nodes ==========

    pub struct BeginPlayNode;
    impl NodeExecutor for BeginPlayNode {
        fn type_id(&self) -> &str { "event.begin_play" }
        fn execute(&self, _ctx: &mut NodeContext) -> NodeResult {
            NodeResult::Continue
        }
    }

    pub struct TickNode;
    impl NodeExecutor for TickNode {
        fn type_id(&self) -> &str { "event.tick" }
        fn execute(&self, ctx: &mut NodeContext) -> NodeResult {
            ctx.set_output("delta_time", Value::Float(ctx.delta_time));
            NodeResult::Continue
        }
    }

    // ========== Print/Debug Node ==========

    pub struct PrintNode;
    impl NodeExecutor for PrintNode {
        fn type_id(&self) -> &str { "debug.print" }
        fn execute(&self, ctx: &mut NodeContext) -> NodeResult {
            let _text = ctx.get_input_string("text").unwrap_or("");
            // In a real implementation, this would log to console
            NodeResult::Continue
        }
    }

    /// Create a registry with all built-in nodes
    pub fn create_default_registry() -> NodeRegistry {
        let mut registry = NodeRegistry::new();

        // Math nodes
        registry.register(
            NodeDef::new("math.add", "Add")
                .category(NodeCategory::Math)
                .input(PinDef::input("a", ValueType::Float))
                .input(PinDef::input("b", ValueType::Float))
                .output(PinDef::output("result", ValueType::Float))
                .pure(),
            Box::new(AddNode),
        );

        registry.register(
            NodeDef::new("math.subtract", "Subtract")
                .category(NodeCategory::Math)
                .input(PinDef::input("a", ValueType::Float))
                .input(PinDef::input("b", ValueType::Float))
                .output(PinDef::output("result", ValueType::Float))
                .pure(),
            Box::new(SubtractNode),
        );

        registry.register(
            NodeDef::new("math.multiply", "Multiply")
                .category(NodeCategory::Math)
                .input(PinDef::input("a", ValueType::Float))
                .input(PinDef::input("b", ValueType::Float))
                .output(PinDef::output("result", ValueType::Float))
                .pure(),
            Box::new(MultiplyNode),
        );

        registry.register(
            NodeDef::new("math.divide", "Divide")
                .category(NodeCategory::Math)
                .input(PinDef::input("a", ValueType::Float))
                .input(PinDef::input("b", ValueType::Float).with_default(Value::Float(1.0)))
                .output(PinDef::output("result", ValueType::Float))
                .pure(),
            Box::new(DivideNode),
        );

        // Flow control
        registry.register(
            NodeDef::new("flow.branch", "Branch")
                .category(NodeCategory::FlowControl)
                .input(PinDef::exec_input("exec_in"))
                .input(PinDef::input("condition", ValueType::Bool))
                .output(PinDef::exec_output("true"))
                .output(PinDef::exec_output("false")),
            Box::new(BranchNode),
        );

        // Logic nodes
        registry.register(
            NodeDef::new("logic.and", "And")
                .category(NodeCategory::Logic)
                .input(PinDef::input("a", ValueType::Bool))
                .input(PinDef::input("b", ValueType::Bool))
                .output(PinDef::output("result", ValueType::Bool))
                .pure(),
            Box::new(AndNode),
        );

        registry.register(
            NodeDef::new("logic.or", "Or")
                .category(NodeCategory::Logic)
                .input(PinDef::input("a", ValueType::Bool))
                .input(PinDef::input("b", ValueType::Bool))
                .output(PinDef::output("result", ValueType::Bool))
                .pure(),
            Box::new(OrNode),
        );

        registry.register(
            NodeDef::new("logic.not", "Not")
                .category(NodeCategory::Logic)
                .input(PinDef::input("value", ValueType::Bool))
                .output(PinDef::output("result", ValueType::Bool))
                .pure(),
            Box::new(NotNode),
        );

        // Comparison nodes
        registry.register(
            NodeDef::new("compare.equals", "Equals")
                .category(NodeCategory::Logic)
                .input(PinDef::input("a", ValueType::Float))
                .input(PinDef::input("b", ValueType::Float))
                .output(PinDef::output("result", ValueType::Bool))
                .pure(),
            Box::new(EqualsNode),
        );

        registry.register(
            NodeDef::new("compare.less_than", "Less Than")
                .category(NodeCategory::Logic)
                .input(PinDef::input("a", ValueType::Float))
                .input(PinDef::input("b", ValueType::Float))
                .output(PinDef::output("result", ValueType::Bool))
                .pure(),
            Box::new(LessThanNode),
        );

        registry.register(
            NodeDef::new("compare.greater_than", "Greater Than")
                .category(NodeCategory::Logic)
                .input(PinDef::input("a", ValueType::Float))
                .input(PinDef::input("b", ValueType::Float))
                .output(PinDef::output("result", ValueType::Bool))
                .pure(),
            Box::new(GreaterThanNode),
        );

        // Event nodes
        registry.register(
            NodeDef::new("event.begin_play", "Begin Play")
                .category(NodeCategory::Event)
                .output(PinDef::exec_output("exec_out"))
                .event()
                .color(1.0, 0.2, 0.2),
            Box::new(BeginPlayNode),
        );

        registry.register(
            NodeDef::new("event.tick", "Tick")
                .category(NodeCategory::Event)
                .output(PinDef::exec_output("exec_out"))
                .output(PinDef::output("delta_time", ValueType::Float))
                .event()
                .color(1.0, 0.2, 0.2),
            Box::new(TickNode),
        );

        // Debug nodes
        registry.register(
            NodeDef::new("debug.print", "Print")
                .category(NodeCategory::Function)
                .input(PinDef::exec_input("exec_in"))
                .input(PinDef::input("text", ValueType::String))
                .output(PinDef::exec_output("exec_out")),
            Box::new(PrintNode),
        );

        registry
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use super::builtin::*;

    #[test]
    fn test_builtin_registry() {
        let registry = create_default_registry();

        assert!(registry.get_definition("math.add").is_some());
        assert!(registry.get_definition("flow.branch").is_some());
        assert!(registry.get_definition("event.begin_play").is_some());
    }

    #[test]
    fn test_graph_execution() {
        let registry = create_default_registry();
        let mut graph = Graph::new("Test");

        // Create a simple graph: BeginPlay -> Print
        let event = graph.add_node("event.begin_play");
        let print = graph.add_node("debug.print");

        graph.add_entry_point(event);

        let _ = graph.connect(
            event, PinId::from_name("exec_out"),
            print, PinId::from_name("exec_in"),
        );

        // Set print text
        if let Some(node) = graph.get_node_mut(print) {
            node.set_pin_value(PinId::from_name("text"), Value::String("Hello!".into()));
        }

        // Execute
        let mut executor = GraphExecutor::new(&graph, &registry);
        let result = executor.execute(event);

        assert!(result.is_ok());
    }
}
